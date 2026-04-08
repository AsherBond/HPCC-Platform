import * as React from "react";
import { makeStyles, Overflow, Breadcrumb, OverflowItem, BreadcrumbItem, BreadcrumbButton, BreadcrumbButtonProps, BreadcrumbDivider, OverflowDivider } from "@fluentui/react-components";
import { bundleIcon, Folder20Filled, Folder20Regular, FolderOpen20Filled, FolderOpen20Regular, } from "@fluentui/react-icons";
import { OverflowMenu } from "./OverflowMenu";

const LineageIcon = bundleIcon(Folder20Filled, Folder20Regular);
const SelectedLineageIcon = bundleIcon(FolderOpen20Filled, FolderOpen20Regular);

const useStyles = makeStyles({
    breadcrumb: {
        margin: 0,
    },
    breadcrumbButton: {
        backgroundColor: "transparent",
        border: 0,
        boxShadow: "none",
        minWidth: "unset",
        height: "auto",
        padding: "0 4px",
    },
});

export interface BreadcrumbInfo {
    id: string;
    label: string;
    props?: BreadcrumbButtonProps
}

interface OverflowGroupDividerProps {
    groupId: string;
}

const OverflowGroupDivider: React.FunctionComponent<OverflowGroupDividerProps> = ({
    groupId,
}) => {
    return <OverflowDivider groupId={groupId}>
        <BreadcrumbDivider data-group={groupId} />
    </OverflowDivider>;
};

function icon(breadcrumb: BreadcrumbInfo, selected: string) {
    return breadcrumb.id === selected ? <SelectedLineageIcon /> : <LineageIcon />;
}

export interface OverflowBreadcrumbProps {
    breadcrumbs: BreadcrumbInfo[];
    selected: string;
    onSelect: (tab: BreadcrumbInfo) => void;
}

export const OverflowBreadcrumb: React.FunctionComponent<OverflowBreadcrumbProps> = ({
    breadcrumbs,
    selected,
    onSelect
}) => {
    const styles = useStyles();

    const overflowItems = React.useMemo(() => {
        return breadcrumbs.map((breadcrumb, idx) => <>
            <OverflowItem id={breadcrumb.id} groupId={breadcrumb.id} key={`button-items-${breadcrumb.id}`}>
                <BreadcrumbItem>
                    <BreadcrumbButton {...breadcrumb.props} className={styles.breadcrumbButton} current={breadcrumb.id === selected} icon={icon(breadcrumb, selected)} onClick={() => onSelect(breadcrumb)}>
                        {breadcrumb.label}
                    </BreadcrumbButton>
                </BreadcrumbItem>
            </OverflowItem>
            {idx < breadcrumbs.length - 1 && <OverflowGroupDivider groupId={breadcrumb.id} />}
        </>);
    }, [breadcrumbs, onSelect, selected, styles.breadcrumbButton]);

    return <Overflow>
        <Breadcrumb className={styles.breadcrumb}>
            {...overflowItems}
            <OverflowMenu menuItems={breadcrumbs.map(breadcrumb => ({ ...breadcrumb, icon: icon(breadcrumb, selected) }))} onMenuSelect={onSelect} />
        </Breadcrumb>
    </Overflow>;
};
